words = ["ant", "bat", "ape"]
p words.each_with_object({}) { |word, h| h[word[0]] = (h[word[0]] || 0) + 1 }
p words.each_with_object([]) { |word, arr| arr << word.upcase }
