$stdout.sync = true

require "rbconfig"

root     = "/tmp/stoned_bundle_eval_gemfile_#{$$}"
app      = File.join(root, "app")
gem_home = File.join(root, "gemhome")
gem_dir  = File.join(root, "mygem")
lib_dir  = File.join(gem_dir, "lib")
[root, app, gem_home, gem_dir, lib_dir].each do |d|
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
File.write(File.join(lib_dir, "mygem.rb"), "module Mygem; end\n")

# Shared gemfile included by main Gemfile
File.write(File.join(app, "shared.gemfile"), <<~GEMFILE)
  gem "mygem", path: #{gem_dir.inspect}
GEMFILE

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  eval_gemfile "shared.gemfile"
GEMFILE

old_gem_home = ENV["GEM_HOME"]
old_home     = ENV["HOME"]
ENV["GEM_HOME"] = gem_home
ENV["HOME"]     = root

ruby_cmd   = RbConfig.ruby.inspect
bundle_exe = File.expand_path("exe/bundle", Dir.pwd)

Dir.chdir(app) do
  out = `#{ruby_cmd} #{bundle_exe.inspect} install 2>&1`.chomp
  puts "--INSTALL--"
  puts out.include?("mygem")
  puts out.include?("Bundle complete!")

  list_out = `#{ruby_cmd} #{bundle_exe.inspect} list 2>&1`.chomp
  puts "--LIST--"
  puts list_out.include?("mygem")
end

ENV["GEM_HOME"] = old_gem_home
ENV["HOME"]     = old_home
system("rm", "-rf", root)
