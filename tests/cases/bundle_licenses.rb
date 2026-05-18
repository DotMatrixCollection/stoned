$stdout.sync = true

require "rbconfig"

root    = "/tmp/stoned_bundle_licenses_#{$$}"
app     = File.join(root, "app")
gem_home = File.join(root, "gemhome")
libA_dir = File.join(root, "liba")
libB_dir = File.join(root, "libb")
[root, app, gem_home, libA_dir, File.join(libA_dir, "lib"),
 libB_dir, File.join(libB_dir, "lib")].each do |d|
  Dir.mkdir(d) unless Dir.exist?(d)
end

File.write(File.join(libA_dir, "liba.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "liba"
    s.version = Gem::Version.new("1.0.0")
    s.summary = "liba"
    s.license = "MIT"
    s.require_paths = ["lib"]
  end
GEMSPEC
File.write(File.join(libA_dir, "lib", "liba.rb"), "module Liba; end\n")

File.write(File.join(libB_dir, "libb.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "libb"
    s.version = Gem::Version.new("2.0.0")
    s.summary = "libb"
    s.license = "Apache-2.0"
    s.require_paths = ["lib"]
  end
GEMSPEC
File.write(File.join(libB_dir, "lib", "libb.rb"), "module Libb; end\n")

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "liba", path: #{libA_dir.inspect}
  gem "libb", path: #{libB_dir.inspect}
GEMFILE

old_gem_home = ENV["GEM_HOME"]
old_home     = ENV["HOME"]
ENV["GEM_HOME"] = gem_home
ENV["HOME"]     = root

ruby_cmd   = RbConfig.ruby.inspect
bundle_exe = File.expand_path("exe/bundle", Dir.pwd)

Dir.chdir(app) do
  `#{ruby_cmd} #{bundle_exe.inspect} install 2>&1`
  puts `#{ruby_cmd} #{bundle_exe.inspect} licenses 2>&1`.chomp
end

ENV["GEM_HOME"] = old_gem_home
ENV["HOME"]     = old_home
system("rm", "-rf", root)
