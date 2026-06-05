words = ["ant", "bear", "cat", "deer"]
p words.group_by { |w| w.length }
p words.partition { |w| w.include?("e") }
