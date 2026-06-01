require "json"

data = {"a" => [1, true, nil]}
p JSON.generate(data)
p JSON.parse(JSON.generate(data))
