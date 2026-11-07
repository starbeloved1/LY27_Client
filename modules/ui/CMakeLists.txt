add_library(ly27_ui STATIC
    include/VideoSurface.hpp
    src/VideoSurface.cpp
    include/DesktopSession.hpp
    src/DesktopSession.cpp
    include/UiTheme.hpp
    src/UiTheme.cpp
    include/MatchNotices.hpp
    src/MatchNotices.cpp
    include/BuffPresentation.hpp
    src/BuffPresentation.cpp
    include/TacticalMapController.hpp
    src/TacticalMapController.cpp
    include/TerritoryOverlayItem.hpp
    src/TerritoryOverlayItem.cpp
    include/BattleAnnouncementItem.hpp
    src/BattleAnnouncementItem.cpp
    include/MapWindow.hpp
    include/TerminalViewModel.hpp
    include/RobotMapModel.hpp
    include/EventListModel.hpp
    src/MapWindow.cpp
    src/TerminalViewModel.cpp
    src/TerminalPlayerHud.cpp
    src/RobotMapModel.cpp
    src/EventListModel.cpp
)
target_include_directories(ly27_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(ly27_ui PUBLIC Qt6::Quick ly27_app)

# 素材和视图随程序编译，运行时不依赖旧项目。
file(GLOB_RECURSE ly27_phase_images CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/images/gamephase/*.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/images/resultpanel/*.png")
file(GLOB ly27_battle_images CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/images/message/*.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/images/robots/*.png")
file(GLOB ly27_buff_images CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/images/buffs/*.png")
file(GLOB_RECURSE ly27_player_images CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/images/crosshair/*.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/images/my_robot/*.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/images/top_robots/*.png")
foreach(existing blue_tab_bar blue_teammate_bg red_tab_bar red_teammate_bg)
    list(REMOVE_ITEM ly27_player_images "${CMAKE_CURRENT_SOURCE_DIR}/resources/images/top_robots/${existing}.png")
endforeach()
qt_add_resources(ly27_ui ly27_ui_resources
    # 图片数据直接写入资源目标文件，避免将动画展开成数百 MB 的 C++ 数组。
    BIG_RESOURCES
    PREFIX "/ly27/ui"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources"
    FILES
        resources/UiText.qml
        resources/BattlefieldMap.qml
        resources/PlayerVideoPage.qml
        resources/PlayerTopBar.qml
        resources/PlayerAimingHud.qml
        resources/PlayerRobotStatus.qml
        resources/PlayerUnitCard.qml
        ${ly27_player_images}
        resources/TerminalSettings.qml
        resources/BattleAnnouncement.qml
        resources/BattleNoticeCard.qml
        resources/EventResultPanel.qml
        resources/AnimatedNotice.qml
        resources/BuffRow.qml
        resources/MatchNotices.qml
        ${ly27_buff_images}
        ${ly27_battle_images}
        resources/BaseHealthBar.qml
        resources/BaseHealthWing.qml
        resources/BattleAnalysisPanel.qml
        resources/DamagePanel.qml
        resources/EventPanel.qml
        resources/FacilityMapMarker.qml
        resources/Map.qml
        resources/MatchHeader.qml
        resources/MatchResources.qml
        resources/MetricComparison.qml
        resources/HudPanel.qml
        resources/TechPanel.qml
        resources/SectionTitle.qml
        resources/MechanismNotices.qml
        resources/MatchPhaseOverlay.qml
        ${ly27_phase_images}
        resources/OutpostStatusBadge.qml
        resources/RobotMarker.qml
        resources/TacticalSymbol.qml
        resources/TacticalMarker.qml
        resources/MapMarkerMenu.qml
        resources/images/injury/sundry_bullet.png
        resources/images/injury/sundry_golf.png
        resources/images/injury/sundry_icon.png
        resources/images/blue_base_blood/blue_base_blood_border.png
        resources/images/blue_base_blood/blue_base_blood_bottom.png
        resources/images/blue_base_blood/blue_base_blood_light.png
        resources/images/blue_base_blood/blue_base_blood_middle.png
        resources/images/blue_base_blood/blue_base_blood_middle_full.png
        resources/images/blue_base_blood/blue_base_blood_top.png
        resources/images/blue_base_blood/blue_base_blood_top_full.png
        resources/images/minimap/blue_map_arrow.png
        resources/images/minimap/blue_map_guard.png
        resources/images/minimap/blue_map_outpost.png
        resources/images/minimap/blue_map_outpost_bg.png
        resources/images/minimap/blue_map_robot_lockbg.png
        resources/images/minimap/map_robot_id_1.png
        resources/images/minimap/map_robot_id_3.png
        resources/images/minimap/map_robot_id_4.png
        resources/images/minimap/red_map_arrow.png
        resources/images/minimap/red_map_guard.png
        resources/images/minimap/red_map_outpost.png
        resources/images/minimap/red_map_outpost_bg.png
        resources/images/minimap/red_map_robot_lockbg.png
        resources/images/minimap/self_map_arrow.png
        resources/images/minimap/self_map_robot.png
        resources/images/red_base_blood/red_base_blood_border.png
        resources/images/red_base_blood/red_base_blood_bottom.png
        resources/images/red_base_blood/red_base_blood_light.png
        resources/images/red_base_blood/red_base_blood_middle.png
        resources/images/red_base_blood/red_base_blood_middle_full.png
        resources/images/red_base_blood/red_base_blood_top.png
        resources/images/red_base_blood/red_base_blood_top_full.png
        resources/images/top_mid/top_middle_background_v2.png
        resources/images/top_mid/red_team_logo.png
        resources/images/top_mid/blue_team_logo.png
        resources/images/top_outpost/blue_outpost_avatar.png
        resources/images/top_outpost/blue_outpost_name.png
        resources/images/top_outpost/red_outpost_avatar.png
        resources/images/top_outpost/red_outpost_name.png
        resources/images/top_robots/blue_tab_bar.png
        resources/images/top_robots/blue_teammate_bg.png
        resources/images/top_robots/red_tab_bar.png
        resources/images/top_robots/red_teammate_bg.png
        resources/maps/battlefield.png
)
