# Hash#dig and Array#dig for safe nested access
config = {
  database: {
    host: "localhost",
    port: 5432,
    credentials: { user: "admin", pass: "secret" }
  },
  cache: { ttl: 300 }
}

p config.dig(:database, :host)
p config.dig(:database, :credentials, :user)
p config.dig(:database, :port)
p config.dig(:cache, :ttl)
p config.dig(:missing, :key)
p config.dig(:database, :missing)

# Array#dig
matrix = [[1, [2, 3]], [4, [5, 6]], [7, [8, 9]]]
p matrix.dig(0, 1, 0)
p matrix.dig(2, 1, 1)
p matrix.dig(1, 0)
p matrix.dig(5)

# Mixed hash/array dig
data = {
  users: [
    { name: "Alice", scores: [95, 87, 92] },
    { name: "Bob",   scores: [78, 82, 88] }
  ]
}
p data.dig(:users, 0, :name)
p data.dig(:users, 1, :scores, 2)
p data.dig(:users, 2, :name)

# dig with nil mid-chain returns nil (doesn't raise)
h = { a: nil }
p h.dig(:a, :b)
