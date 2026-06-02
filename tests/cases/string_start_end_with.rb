# String#start_with?, end_with?, include?
p "hello world".start_with?("hello")
p "hello world".start_with?("world")
p "hello world".start_with?("foo", "hello")  # any match
p "hello world".end_with?("world")
p "hello world".end_with?("hello")
p "hello world".end_with?("foo", "rld", "xyz")
p "hello world".include?("lo wo")
p "hello world".include?("xyz")

# Case sensitivity
p "Hello".start_with?("hello")
p "Hello".start_with?("Hello")

# Empty string edge cases
p "hello".start_with?("")
p "hello".end_with?("")
p "".start_with?("")

# Practical: file extension check
filenames = ["app.rb", "test.rb", "main.py", "readme.md"]
p filenames.select { |f| f.end_with?(".rb") }
p filenames.reject { |f| f.start_with?("test") }

# Combined
urls = ["https://example.com", "http://foo.com", "ftp://bar.org"]
p urls.select { |u| u.start_with?("https://", "http://") }
p urls.count { |u| u.include?(".com") }
