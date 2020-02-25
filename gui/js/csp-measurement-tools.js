/* global IApi, CosmoScout */

/**
 * Measurement Tools
 */
class MeasurementToolsApi extends IApi {
  /**
   * @inheritDoc
   */
  name = 'measurementTools';

  /**
   * @inheritDoc
   */
  init() {
    CosmoScout.gui.initSlider('set_widget_scale', 0.2, 2.0, 0.01, [0.6]);
  }

  /**
   * TODO
   * @param name {string}
   * @param icon {string}
   */
  // eslint-disable-next-line class-methods-use-this
  add(name, icon) {
    const area = document.getElementById('measurement-tools');

    const tool = CosmoScout.gui.loadTemplateContent('measurement-tools');

    tool.innerHTML = tool.innerHTML
      .replace(/%CONTENT%/g, name)
      .replace(/%ICON%/g, icon)
      .trim();

    tool.addEventListener('change', (event) => {
      document.querySelectorAll('#measurement-tools .radio-button').forEach((node) => {
        if (node.id !== `set_tool_${icon}`) {
          node.checked = false;
        }
      });

      if (event.target.checked) {
        CosmoScout.callbacks.set_measurement_tool(name);
      } else {
        CosmoScout.callbacks.set_measurement_tool('none');
      }
    });

    area.appendChild(tool);
  }

  /**
   * Deselect all measurement tools
   */
  // eslint-disable-next-line class-methods-use-this
  deselect() {
    document.querySelectorAll('#measurement-tools .radio-button').forEach((node) => {
      node.checked = false;
    });
  }
}

// Init class on load
(() => {
  CosmoScout.init(MeasurementToolsApi);
})();
