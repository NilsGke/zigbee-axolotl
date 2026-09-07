const fz = require("zigbee-herdsman-converters/converters/fromZigbee");
const tz = require("zigbee-herdsman-converters/converters/toZigbee");
const reporting = require("zigbee-herdsman-converters/lib/reporting");
const {
  presets: e,
  access: ea,
} = require("zigbee-herdsman-converters/lib/exposes");

const MODES = ["white", "yellow", "orange", "pink", "blue", "cycling"];

module.exports = {
  zigbeeModel: ["Axolotl"],
  model: "Axolotl",
  vendor: "NilsGke",
  description: "Minecraft Axolotl Lamp",
  fromZigbee: [
    fz.on_off,
    {
      cluster: 64512, // 0xFC00
      type: ["attributeReport", "readResponse"],
      convert: (_model, msg) => {
        if (msg.data["1"] === undefined) return {};
        return { mode: MODES[msg.data["1"]] };
      },
    },
  ],
  toZigbee: [
    tz.on_off,
    {
      key: ["mode"],
      convertSet: async (entity, _key, value, _meta) => {
        const idx = MODES.indexOf(value);
        if (idx < 0) throw new Error(`mode must be one of ${MODES.join(", ")}`);
        await entity.write(0xfc00, { 0x0001: { value: idx, type: 0x30 } });
        return { state: { mode: value } };
      },
      convertGet: async (entity) => {
        await entity.read(0xfc00, [0x0001]);
      },
    },
  ],
  exposes: [e.switch(), e.enum("mode", ea.ALL, MODES)],
  configure: async (device, coordinatorEndpoint) => {
    const ep = device.getEndpoint(10);
    await reporting.bind(ep, coordinatorEndpoint, ["genOnOff"]);
    await reporting.onOff(ep);
  },
};
