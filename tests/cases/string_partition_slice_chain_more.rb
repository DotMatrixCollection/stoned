text = "path/to/file.rb"

p text.partition("/")
p text.rpartition("/")
p text[0, 4]
p text[-7..-1]
p text.split("/").join("::")
