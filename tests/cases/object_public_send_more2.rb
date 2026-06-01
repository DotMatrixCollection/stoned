obj = "abc"
p obj.public_send(:upcase)
p obj.send(:length)
