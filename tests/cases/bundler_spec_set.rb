$stdout.sync = true

require "bundler/spec_set"
require "bundler/lockfile_parser"

parser = Bundler::LockfileParser.new(<<~LOCK)
  GEM
    remote: https://rubygems.org/
    specs:
      alpha (1.2.3)
      beta (2.0.0)
LOCK

specs = Bundler::SpecSet.new(parser.specs)
puts specs.length
puts specs.size
puts specs.empty? == false
puts specs[0].name
puts specs[1].version
puts specs.to_a.length
puts specs.names.join(",")
alpha_only = specs.for([Struct.new(:name).new("alpha")])
puts alpha_only.length
puts specs.to_hash["beta"].version
names = []
specs.each { |spec| names << spec.name }
puts names.join(",")
