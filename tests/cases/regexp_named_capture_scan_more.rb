text = "x=1 y=22"
p text.scan(/(?<key>[a-z])=(?<val>\d+)/)
m = /(?<key>[a-z])=(?<val>\d+)/.match("z=333")
p [m[:key], m[:val]]
