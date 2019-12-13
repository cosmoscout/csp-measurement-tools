init_slider("set_widget_scale", 0.2, 2.0, 0.01, [0.6]);

function add_measurement_tool(name, icon) {
    return CosmoScout.call('sidebar', 'addMeasurementTool', name, icon);
}

function deselect_measurement_tool() {
    return CosmoScout.call('sidebar', 'deselectMeasurementTool');
}