$stdout.sync = true

require "bundler/source_list"
require "bundler/source/rubygems"
require "bundler/source/path"
require "bundler/source/git"

list = Bundler::SourceList.new
rubygems = Bundler::Source::Rubygems.new(remote: "https://rubygems.org")
path = Bundler::Source::Path.new(path: "/tmp/demo")
git = Bundler::Source::Git.new(git: "https://github.com/demo/repo.git", branch: "main", ref: "abc123")

list.add_source(rubygems)
list.add_source(path)
list.add_source(git)

puts list.all_sources.length
puts list.rubygems_sources.length
puts list.path_sources.length
puts list.git_sources.length
puts list.default_source.to_s
puts list.path_sources[0].to_s
puts list.git_sources[0].to_s
puts list.git_sources[0].branch
puts list.git_sources[0].ref
