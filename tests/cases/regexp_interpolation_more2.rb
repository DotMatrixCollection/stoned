r = /a#{"b"}c/
p r.match("abc").to_s
p r.source
