$stdout.sync = true

require "bundler/source"
require "bundler/source_list"

list = Bundler::SourceList.new
metadata = Bundler::Source::Metadata.new
installed = Bundler::Source::Installed.new(["a", "b"])

list.add_source(metadata)
list.add_source(installed)

puts Bundler::Source::Metadata.is_a?(Class)
puts Bundler::Source::Installed.is_a?(Class)
puts list.metadata_source.to_s
puts list.installed_source.to_s
puts list.installed_source.specs.length
