$stdout.sync = true

require "rbconfig"

root     = "/tmp/stoned_bundle_doctor_#{$$}"
app      = File.join(root, "app")
gem_home = File.join(root, "gemhome")
lib_dir  = File.join(root, "mygem", "lib")
[root, app, gem_home, File.join(root, "mygem"), lib_dir].each do |d|
  Dir.mkdir(d) unless Dir.exist?(d)
end

File.write(File.join(root, "mygem", "mygem.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "mygem"
    s.version = Gem::Version.new("1.0.0")
    s.summary = "mygem"
    s.require_paths = ["lib"]
  end
GEMSPEC
File.write(File.join(lib_dir, "mygem.rb"), "module Mygem; end\n")

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "mygem", path: #{File.join(root, "mygem").inspect}
GEMFILE

old_gem_home = ENV["GEM_HOME"]
old_home     = ENV["HOME"]
ENV["GEM_HOME"] = gem_home
ENV["HOME"]     = root

ruby_cmd   = RbConfig.ruby.inspect
bundle_exe = File.expand_path("exe/bundle", Dir.pwd)

Dir.chdir(app) do
  # After install, doctor should say the bundle is satisfied
  `#{ruby_cmd} #{bundle_exe.inspect} install 2>&1`
  out = `#{ruby_cmd} #{bundle_exe.inspect} doctor 2>&1`.chomp
  puts "--HEALTHY--"
  puts out
  puts $?.exitstatus

  # Without a lockfile at all
  File.delete("Gemfile.lock") if File.exist?("Gemfile.lock")
  out2 = `#{ruby_cmd} #{bundle_exe.inspect} doctor 2>&1`.chomp
  puts "--NO-LOCK--"
  puts out2.include?("Run `bundle install`")
  puts $?.exitstatus
end

ENV["GEM_HOME"] = old_gem_home
ENV["HOME"]     = old_home
system("rm", "-rf", root)
