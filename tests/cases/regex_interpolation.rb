# /#{expr}/ interpolated regex literal
name = "world"
re = /hello #{name}/
puts re === "hello world"
puts re === "hello ruby"

# Flags preserved
word = "test"
ri = /#{word}/i
puts ri === "TEST"

# Anchors and complex patterns
prefix = "user"
re2 = /\A#{prefix}_\d+\z/
puts re2 === "user_42"
puts re2 === "admin_42"

# Named captures via interpolation
seg = "([^/]+)"
route = /\A\/users\/#{seg}\/posts\/#{seg}\z/
m = route.match("/users/10/posts/5")
puts m[1]
puts m[2]

# Interpolation doesn't break character classes
chars = "abc"
re3 = /[#{chars}]+/
puts re3 === "aabbcc"
puts re3 === "xyz"
