$stdout.sync = true

require "bundler/source_list"
require "bundler/source/rubygems"

list = Bundler::SourceList.new
source = Bundler::Source::Rubygems.new(remotes: ["https://rubygems.org", "https://example.test"])
list.add_source(source)

puts list.all_sources.length
puts list.rubygems_sources.length
puts list.rubygems_sources[0].to_s
