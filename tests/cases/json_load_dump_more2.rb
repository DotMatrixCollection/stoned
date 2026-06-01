require "json"

text = JSON.dump({"x" => 1})
p text
p JSON.load(text)["x"]
