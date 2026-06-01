require "json"

data = JSON.parse("{\"a\":1,\"b\":[true,null]}")
p data
puts JSON.generate({a: 1, b: [true, nil]})
