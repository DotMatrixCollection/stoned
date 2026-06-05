words = ["", "ruby", "", "stoned"]
p words.select(&:empty?)
p words.reject(&:empty?)
