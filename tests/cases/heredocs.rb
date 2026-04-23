# Non-interpolating, no squiggly
x = <<'HEREDOC'
hello
world
HEREDOC
print x

# Non-interpolating, squiggly
y = <<~'HEREDOC'
  no #{interpolation} here
HEREDOC
print y

# Interpolating, no squiggly
lang = "stoned"
w = <<"HEREDOC"
running #{lang}
HEREDOC
print w

# Interpolating, squiggly
name = "Ruby"
z = <<~HEREDOC
  Hello, #{name}!
  Goodbye.
HEREDOC
print z

# Expression in interpolation
a = 1
b = 2
msg = <<~HEREDOC
  sum is #{a + b}
HEREDOC
print msg

# Sequential heredocs
first = <<~A
  one
A
second = <<~B
  two
B
print first
print second

# Empty heredoc body
empty = <<~HEREDOC
HEREDOC
puts empty.length

# Heredoc assigned and used as method receiver
upcased = <<~HEREDOC
  hello world
HEREDOC
print upcased.upcase
