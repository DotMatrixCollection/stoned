rx = /foo/
p rx === "food"
p rx === "bar"
p ["foo", "bar", "seafood"].select { |s| rx === s }
