words = ["ant", "bat", "ant", "cat", "bat", "ant"]
p words.tally
p words.tally.sort_by { |pair| [-pair[1], pair[0]] }
