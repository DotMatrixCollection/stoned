$stdout.sync = true

require "rbconfig"

root     = "/tmp/stoned_bundler_inline_#{$$}"
gem_home = File.join(root, "gemhome")
gem_dir  = File.join(root, "mygem")
lib_dir  = File.join(gem_dir, "lib")
[root, gem_home, gem_dir, lib_dir].each do |d|
  Dir.mkdir(d) unless Dir.exist?(d)
end

File.write(File.join(gem_dir, "mygem.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "mygem"
    s.version = Gem::Version.new("1.0.0")
    s.summary = "mygem"
    s.require_paths = ["lib"]
  end
GEMSPEC
File.write(File.join(lib_dir, "mygem.rb"), "INLINE_GEM_LOADED = true\n")

script_path = File.join(root, "use_inline.rb")
File.write(script_path, <<~RUBY)
  require "bundler/inline"
  gemfile(quiet: true) do |g|
    g.source "https://rubygems.org"
    g.gem "mygem", path: #{gem_dir.inspect}
  end
  require "mygem"
  puts INLINE_GEM_LOADED
RUBY

old_gem_home = ENV["GEM_HOME"]
old_home     = ENV["HOME"]
ENV["GEM_HOME"] = gem_home
ENV["HOME"]     = root

out = `#{RbConfig.ruby.inspect} #{script_path.inspect} 2>&1`.chomp
puts "--INLINE--"
puts out

ENV["GEM_HOME"] = old_gem_home
ENV["HOME"]     = old_home
system("rm", "-rf", root)
