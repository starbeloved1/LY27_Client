add_library(ly27_ui STATIC
    include/BattleAnnouncementItem.hpp
    src/BattleAnnouncementItem.cpp
    include/MapWindow.hpp
    include/TerminalViewModel.hpp
    include/RobotMapModel.hpp
    include/EventListModel.hpp
    src/MapWindow.cpp
    src/TerminalViewModel.cpp
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
qt_add_resources(ly27_ui ly27_ui_resources
    PREFIX "/ly27/ui"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources"
    FILES
        resources/BattleAnnouncement.qml
        ${ly27_battle_images}
        resources/BaseHealthBar.qml
        resources/BaseHealthWing.qml
        resources/BattleAnalysisPanel.qml
        resources/EventPanel.qml
        resources/FacilityMapMarker.qml
        resources/Map.qml
        resources/MatchHeader.qml
        resources/MatchResources.qml
        resources/MetricComparison.qml
        resources/HudPanel.qml
        resources/MechanismNotices.qml
        resources/MatchPhaseOverlay.qml
        ${ly27_phase_images}
        resources/OutpostStatusBadge.qml
        resources/RobotMarker.qml
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
