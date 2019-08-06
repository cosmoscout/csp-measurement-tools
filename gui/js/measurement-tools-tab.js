init_slider("set_widget_scale", 0.2, 2.0, 0.01, [0.8]);

function add_measurement_tool(name, icon) {
    const area = $('#measurement-tools');
    area.append('<div class=\'col s4 center\'>\
                    <label>\
                        <input id=\'set_tool_' +
        icon + '\' type=\'checkbox\' name=\'measurement-tool\' class=\'radio-button\' />\
                        <div class=\'waves-effect waves-light block btn btn-large glass\'>\
                            <i class=\'material-icons\'>' +
        icon + '</i> ' + name + '\
                        </div>\
                    </label>\
                </div>');

    $('#set_tool_' + icon).change(function () {
        $('#measurement-tools .radio-button').not('#set_tool_' + icon).prop('checked', false);

        if (this.checked) {
            window.call_native('set_measurement_tool', name);
        } else {
            window.call_native('set_measurement_tool', 'none');
        }
    });
}

function deselect_measurement_tool() {
    $('#measurement-tools .radio-button').prop('checked', false);
}