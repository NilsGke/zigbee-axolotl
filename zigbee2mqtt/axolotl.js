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
    {
      cluster: 64512, // 0xFC00
      type: ["attributeReport", "readResponse"],
      convert: (_model, msg) => {
        const out = {};
        if (msg.data["0"] !== undefined) {
          out.state = msg.data["0"] ? "ON" : "OFF";
        }
        if (msg.data["1"] !== undefined) {
          out.mode = MODES[msg.data["1"]];
        }
        return out;
      },
    },
  ],
  toZigbee: [
    {
      key: ["state", "mode"],
      convertSet: async (entity, key, value, _meta) => {
        if (key === "state") {
          await entity.write(0xfc00, {
            0x0000: { value: value === "ON" ? 1 : 0, type: 0x10 },
          });
          return { state: { state: value } };
        } else {
          await entity.write(0xfc00, {
            0x0001: { value: MODES.indexOf(value), type: 0x30 },
          });
          return { state: { mode: value } };
        }
      },
      convertGet: async (entity) => {
        await entity.read(0xfc00, [0x0000, 0x0001]);
      },
    },
  ],
  exposes: [
    e.binary("state", ea.ALL, "ON", "OFF"),
    e.enum("mode", ea.ALL, MODES),
  ],
};
