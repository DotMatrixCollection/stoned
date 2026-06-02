h = {"name" => "alice", "score" => "42"}
p h.transform_keys(&:to_sym)
p h.transform_values(&:upcase)

config = {debug: false, verbose: true, timeout: 30}
p config.transform_keys(&:to_s)
p config.reject { |_, v| v == false }.keys
