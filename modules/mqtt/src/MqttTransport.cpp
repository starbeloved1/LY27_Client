#include "MqttTransport.hpp"

#include <MQTTAsync.h>
#include <QDateTime>
#include <QMetaObject>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <algorithm>
#include <chrono>
#include <functional>

namespace ly27 {

struct MqttTransport::Private {
    struct Context {
        MqttTransport *owner;
        quint64 generation;
        int subscriptionCount;
    };
    MqttTransport *owner;
    MQTTAsync client = nullptr;
    std::unique_ptr<Context> context;
    QTimer retry;
    NetworkConfig config;
    QStringList topics;
    QString state = "stopped";
    QString lastError;
    quint64 generation = 0;
    int attempts = 0;
    int successfulConnections = 0;
    int subscribed = 0;
    int retryMs = 1000;
    qint64 received = 0;
    qint64 delivered = 0;

    explicit Private(MqttTransport *value) : owner(value)
    {
        retry.setSingleShot(true);
        QObject::connect(&retry, &QTimer::timeout, owner, [this]() {
            if (client && MQTTAsync_isConnected(client)) subscribe();
            else connect();
        });
    }

    void setState(const QString &value)
    {
        if (state == value) return;
        state = value;
        emit owner->stateChanged(state);
    }

    void report(const QString &message)
    {
        lastError = message;
        emit owner->errorOccurred(message);
    }

    void scheduleRetry(const QString &message)
    {
        subscribed = 0;
        report(message);
        setState("reconnecting");
        retry.start(retryMs);
        retryMs = std::min(retryMs * 2, 30000);
    }

    // Paho 回调只复制数据并投递；对象销毁前先销毁客户端，使回调上下文保持有效。
    static void post(Context *context, std::function<void(Private &)> action)
    {
        auto *target = context->owner;
        const auto session = context->generation;
        QMetaObject::invokeMethod(target, [target, session, action = std::move(action)]() {
            auto &self = *target->d;
            if (session == self.generation && self.client) action(self);
        }, Qt::QueuedConnection);
    }

    static void connected(void *context, MQTTAsync_successData *)
    {
        post(static_cast<Context *>(context), [](Private &self) {
            ++self.successfulConnections;
            self.lastError.clear();
            self.retryMs = 1000;
            self.subscribe();
        });
    }

    static void connectFailed(void *context, MQTTAsync_failureData *failure)
    {
        const int code = failure ? failure->code : MQTTASYNC_FAILURE;
        post(static_cast<Context *>(context), [code](Private &self) {
            const auto message = QStringLiteral("MQTT 连接失败，错误码 %1").arg(code);
            // 身份、认证或协议被拒绝时保留诊断，避免无限重复错误配置。
            if (code == 1 || code == 2 || code == 4 || code == 5) {
                self.report(message);
                self.setState("error");
            } else self.scheduleRetry(message);
        });
    }

    static void lost(void *context, char *cause)
    {
        const auto reason = cause ? QString::fromUtf8(cause) : QStringLiteral("连接中断");
        post(static_cast<Context *>(context), [reason](Private &self) { self.scheduleRetry(reason); });
    }

    static void subscribedCallback(void *context, MQTTAsync_successData *response)
    {
        auto *ctx = static_cast<Context *>(context);
        bool accepted = response != nullptr;
        if (response) {
            for (int i = 0; i < ctx->subscriptionCount; ++i) {
                const int qos = ctx->subscriptionCount == 1 ? response->alt.qos : response->alt.qosList[i];
                if (qos < 0 || qos > 1) accepted = false;
            }
        }
        post(ctx, [accepted](Private &self) {
            if (!accepted) {
                self.report("服务器拒绝了至少一个订阅，未进入就绪状态");
                self.setState("error");
                return;
            }
            self.subscribed = self.topics.size();
            self.setState("ready");
        });
    }

    static void subscribeFailed(void *context, MQTTAsync_failureData *failure)
    {
        const int code = failure ? failure->code : MQTTASYNC_FAILURE;
        post(static_cast<Context *>(context), [code](Private &self) {
            self.scheduleRetry(QStringLiteral("MQTT 订阅失败，错误码 %1").arg(code));
        });
    }

    static int arrived(void *context, char *topic, int topicLength, MQTTAsync_message *message)
    {
        auto *ctx = static_cast<Context *>(context);
        const auto name = topicLength ? QString::fromUtf8(topic, topicLength) : QString::fromUtf8(topic);
        // 限制回调复制量；超长数据不投递到业务事件循环。
        if (message->payloadlen > 1024 * 1024 || message->payloadlen < 0) {
            post(ctx, [](Private &self) { self.report("MQTT 消息超过 1 MiB 接收上限"); });
        } else {
            const QByteArray payload(static_cast<const char *>(message->payload), message->payloadlen);
            const auto receivedUtc = QDateTime::currentMSecsSinceEpoch();
            const int qos = message->qos;
            const bool retained = message->retained != 0;
            // 单调时间在回调线程独立读取，使用进程共享的稳态时钟纪元。
            const auto stamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            post(ctx, [name, payload, receivedUtc, stamp, qos, retained](Private &self) {
                ++self.received;
                emit self.owner->messageReceived(name, payload, receivedUtc, stamp, qos, retained);
            });
        }
        MQTTAsync_freeMessage(&message);
        MQTTAsync_free(topic);
        return 1;
    }

    static void published(void *context, MQTTAsync_successData *response)
    {
        const int token = response ? response->token : 0;
        post(static_cast<Context *>(context), [token](Private &self) {
            ++self.delivered;
            emit self.owner->publishDelivered(token);
        });
    }

    static void publishFailed(void *context, MQTTAsync_failureData *failure)
    {
        const int code = failure ? failure->code : MQTTASYNC_FAILURE;
        const int token = failure ? failure->token : 0;
        post(static_cast<Context *>(context), [code, token](Private &self) {
            const auto error = QStringLiteral("MQTT 发布失败，错误码 %1").arg(code);
            self.report(error);
            emit self.owner->publishFailed(token, error);
        });
    }

    void connect()
    {
        if (!client) return;
        ++attempts;
        setState("connecting");
        MQTTAsync_connectOptions options = MQTTAsync_connectOptions_initializer;
        options.keepAliveInterval = 10;
        options.cleansession = 1;
        options.connectTimeout = 2;
        options.maxInflight = 32;
        options.MQTTVersion = MQTTVERSION_3_1_1;
        options.automaticReconnect = 0;
        options.onSuccess = connected;
        options.onFailure = connectFailed;
        options.context = context.get();
        const int result = MQTTAsync_connect(client, &options);
        if (result != MQTTASYNC_SUCCESS)
            scheduleRetry(QStringLiteral("无法提交 MQTT 连接请求，错误码 %1").arg(result));
    }

    void subscribe()
    {
        if (topics.isEmpty()) { setState("ready"); return; }
        setState("subscribing");
        QList<QByteArray> names;
        QList<char *> pointers;
        QList<int> qos(topics.size(), 1);
        for (const auto &topic : topics) names.append(topic.toUtf8());
        for (auto &name : names) pointers.append(name.data());
        MQTTAsync_responseOptions options = MQTTAsync_responseOptions_initializer;
        options.context = context.get();
        options.onSuccess = subscribedCallback;
        options.onFailure = subscribeFailed;
        const int result = MQTTAsync_subscribeMany(client, topics.size(), pointers.data(), qos.data(), &options);
        if (result != MQTTASYNC_SUCCESS) {
            report(QStringLiteral("无法提交 MQTT 订阅请求，错误码 %1").arg(result));
            setState("error");
        }
    }
};

MqttTransport::MqttTransport(QObject *parent) : QObject(parent), d(std::make_unique<Private>(this)) {}
MqttTransport::~MqttTransport() { stop(); }

bool MqttTransport::start(const NetworkConfig &config, const QStringList &topics)
{
    Q_ASSERT(QThread::currentThread() == thread());
    stop();
    d->config = config;
    d->topics = topics;
    d->attempts = d->successfulConnections = d->subscribed = 0;
    d->received = d->delivered = 0;
    d->retryMs = 1000;
    d->lastError.clear();
    if (config.mqttClientId.isEmpty() || config.mqttClientId.contains(QChar(0))) {
        d->report("启用 MQTT 前必须配置有效的 network.mqtt_client_id");
        d->setState("error");
        return false;
    }
    QUrl url;
    url.setScheme("tcp"); url.setHost(config.mqttHost); url.setPort(config.mqttPort);
    if (!url.isValid() || url.host().isEmpty()) {
        d->report("无效的 MQTT 主机地址"); d->setState("error"); return false;
    }
    for (const auto &topic : topics) {
        if (topic.isEmpty() || topic.contains(QChar(0)) || topic.contains('#') || topic.contains('+')) {
            d->report("订阅列表必须使用明确的 Topic"); d->setState("error"); return false;
        }
    }
    const auto address = url.toEncoded();
    const auto identity = config.mqttClientId.toUtf8();
    const int result = MQTTAsync_create(&d->client, address.constData(), identity.constData(),
                                       MQTTCLIENT_PERSISTENCE_NONE, nullptr);
    if (result != MQTTASYNC_SUCCESS) {
        d->report(QStringLiteral("创建 MQTT 客户端失败，错误码 %1").arg(result));
        d->setState("error"); return false;
    }
    d->context = std::make_unique<Private::Context>(Private::Context{this, d->generation, int(topics.size())});
    const int callbacks = MQTTAsync_setCallbacks(d->client, d->context.get(), Private::lost, Private::arrived, nullptr);
    if (callbacks != MQTTASYNC_SUCCESS) {
        stop(); d->report("注册 MQTT 回调失败"); d->setState("error"); return false;
    }
    d->connect();
    return true;
}

void MqttTransport::stop()
{
    Q_ASSERT(QThread::currentThread() == thread());
    ++d->generation;
    d->retry.stop();
    if (d->client) {
        MQTTAsync_disconnectOptions options = MQTTAsync_disconnectOptions_initializer;
        options.timeout = 0;
        MQTTAsync_disconnect(d->client, &options);
        // 不在 Paho 回调中销毁；返回后才释放本代回调上下文。
        MQTTAsync_destroy(&d->client);
    }
    d->context.reset();
    d->subscribed = 0;
    d->setState("stopped");
}

bool MqttTransport::publish(const QString &topic, const QByteArray &payload, int qos, int *token)
{
    Q_ASSERT(QThread::currentThread() == thread());
    if (token) *token = 0;
    if (!d->client || d->state != "ready") { d->report("MQTT 未就绪，拒绝发布且不缓存指令"); return false; }
    if (topic.isEmpty() || topic.contains(QChar(0)) || topic.contains('#') || topic.contains('+')
        || payload.size() > 1024 * 1024 || qos < 0 || qos > 1) {
        d->report("发布 Topic、载荷长度或 QoS 不合法"); return false;
    }
    MQTTAsync_message message = MQTTAsync_message_initializer;
    message.payload = const_cast<char *>(payload.constData());
    message.payloadlen = payload.size(); message.qos = qos; message.retained = 0;
    MQTTAsync_responseOptions options = MQTTAsync_responseOptions_initializer;
    options.context = d->context.get(); options.onSuccess = Private::published; options.onFailure = Private::publishFailed;
    const auto name = topic.toUtf8();
    const int result = MQTTAsync_sendMessage(d->client, name.constData(), &message, &options);
    if (result != MQTTASYNC_SUCCESS) { d->report(QStringLiteral("无法提交发布，错误码 %1").arg(result)); return false; }
    if (token) *token = options.token;
    emit publishQueued(topic, options.token);
    return true;
}

QJsonObject MqttTransport::snapshot() const
{
    return {{"state", d->state}, {"attempts", d->attempts},
            {"successful_connections", d->successfulConnections}, {"subscribed_topics", d->subscribed},
            {"received_messages", d->received}, {"delivered_messages", d->delivered}, {"last_error", d->lastError}};
}

} // 命名空间 ly27
