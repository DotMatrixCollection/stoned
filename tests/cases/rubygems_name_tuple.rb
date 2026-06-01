require "rubygems"

nt = Gem::NameTuple.new("demo", "1.2.3")
p nt.spec_name
p nt.full_name
p nt.lock_name
