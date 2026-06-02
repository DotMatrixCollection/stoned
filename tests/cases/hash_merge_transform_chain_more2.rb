defaults = {host: "localhost", port: 3000}
override = {port: 9292, debug: true}

p defaults.merge(override)
p defaults.merge(override) { |key, old_value, new_value| "#{key}:#{old_value}->#{new_value}" }
p override.transform_keys { |key| key.to_s }
p override.transform_values { |value| value == true ? "yes" : value }
