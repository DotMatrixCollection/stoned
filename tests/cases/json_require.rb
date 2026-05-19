require 'json'

p JSON.parse('42')
p JSON.parse('"hello"')
p JSON.parse('true')
p JSON.parse('null')
p JSON.parse('[1,2,3]')

data = JSON.parse('{"name":"Alice","age":30}')
p data["name"]
p data["age"]

p JSON.generate({name: "Bob", scores: [1,2]})
p JSON.generate(nil)
p JSON.generate([true, false])

begin
  JSON.parse('{bad}')
rescue JSON::ParseError => e
  p e.class
end
