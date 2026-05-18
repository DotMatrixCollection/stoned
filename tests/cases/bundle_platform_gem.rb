$stdout.sync = true

require "rbconfig"

root     = "/tmp/stoned_bundle_platform_gem_#{$$}"
app      = File.join(root, "app")
gem_home = File.join(root, "gemhome")
posix_dir = File.join(root, "posixgem")
win_dir   = File.join(root, "wingem")
[root, app, gem_home,
 posix_dir, File.join(posix_dir, "lib"),
 win_dir,   File.join(win_dir,   "lib")].each do |d|
  Dir.mkdir(d) unless Dir.exist?(d)
end

File.write(File.join(posix_dir, "posixgem.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "posixgem"
    s.version = Gem::Version.new("1.0.0")
    s.summary = "posixgem"
    s.require_paths = ["lib"]
  end
GEMSPEC
File.write(File.join(posix_dir, "lib", "posixgem.rb"), "module Posixgem; end\n")

File.write(File.join(win_dir, "wingem.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "wingem"
    s.version = Gem::Version.new("1.0.0")
    s.summary = "wingem"
    s.require_paths = ["lib"]
  end
GEMSPEC
File.write(File.join(win_dir, "lib", "wingem.rb"), "module Wingem; end\n")

# Gemfile uses platforms: to target specific platforms
File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "posixgem", path: #{posix_dir.inspect}, platforms: :ruby
  gem "wingem",   path: #{win_dir.inspect},   platforms: :mswin
GEMFILE

old_gem_home = ENV["GEM_HOME"]
old_home     = ENV["HOME"]
ENV["GEM_HOME"] = gem_home
ENV["HOME"]     = root

ruby_cmd   = RbConfig.ruby.inspect
bundle_exe = File.expand_path("exe/bundle", Dir.pwd)

Dir.chdir(app) do
  `#{ruby_cmd} #{bundle_exe.inspect} install 2>&1`

  list_out = `#{ruby_cmd} #{bundle_exe.inspect} list 2>&1`.chomp
  puts "--LIST--"
  puts list_out.include?("posixgem")
  puts list_out.include?("wingem")
end

ENV["GEM_HOME"] = old_gem_home
ENV["HOME"]     = old_home
system("rm", "-rf", root)
