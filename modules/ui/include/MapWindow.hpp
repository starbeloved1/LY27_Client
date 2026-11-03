#pragma once

#include <QQuickView>

namespace ly27 {

class TerminalViewModel;

class MapWindow final : public QQuickView {
public:
    explicit MapWindow(TerminalViewModel *model = nullptr, QWindow *parent = nullptr);
};

}
