class MeasurementToolApi extends IApi {
    name = 'measurementTool';

    /**
     * TODO
     * @param name {string}
     * @param icon {string}
     */
    addMeasurementTool(name, icon) {
        const area = document.getElementById('measurement-tools');

        const tool = CosmoScout.loadTemplateContent('measurement-tools');

        tool.innerHTML = tool.innerHTML
            .replace(this.regex('CONTENT'), name)
            .replace(this.regex('ICON'), icon)
            .trim();

        tool.addEventListener('click', () => {
            CosmoScout.callNative('set_celestial_body', name);
        });

        tool.addEventListener('change', (event) => {
            document.querySelectorAll('#measurement-tools .radio-button').forEach((node) => {
                if (node.id !== `set_tool_${icon}`) {
                    node.checked = false;
                }
            });

            if (event.target.checked) {
                CosmoScout.callNative('set_measurement_tool', name);
            } else {
                CosmoScout.callNative('set_measurement_tool', 'none');
            }
        });

        area.appendChild(tool);
    }

    /**
     * Deselect all measurement tools
     */
    deselectMeasurementTool() {
        document.querySelectorAll('#measurement-tools .radio-button').forEach((node) => {
            node.checked = false;
        });
    }
}

CosmoScout.init(MeasurementToolApi);

CosmoScout.initSlider("set_widget_scale", 0.2, 2.0, 0.01, [0.6]);
