require "json"

p JSON.parse("[1,{\"x\":false},null]")
p JSON.generate([1, {x: false}, nil])
