$stdout.sync = true

require "rbconfig"

root     = "/tmp/stoned_bundle_install_local_#{$$}"
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

# Gemfile with only a path gem — --local should always succeed since no network needed
File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "mygem", path: #{gem_dir.inspect}
GEMFILE

# Gemfile with a remote gem — --local without vendor/cache should fail
File.write(File.join(app, "Gemfile.remote"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "rack"
GEMFILE

old_gem_home = ENV["GEM_HOME"]
old_home     = ENV["HOME"]
ENV["GEM_HOME"] = gem_home
ENV["HOME"]     = root

ruby_cmd   = RbConfig.ruby.inspect
bundle_exe = File.expand_path("exe/bundle", Dir.pwd)

Dir.chdir(app) do
  # Path-only bundle: --local succeeds without vendor/cache
  out = `#{ruby_cmd} #{bundle_exe.inspect} install --local 2>&1`.chomp
  puts "--PATH-ONLY-LOCAL--"
  puts out.include?("Bundle complete!")
  puts $?.exitstatus

  # Remote gem + no vendor/cache: --local should fail
  ENV["BUNDLE_GEMFILE"] = File.join(app, "Gemfile.remote")
  out2 = `BUNDLE_GEMFILE=#{File.join(app, "Gemfile.remote").inspect} #{ruby_cmd} #{bundle_exe.inspect} install --local 2>&1`.chomp
  puts "--REMOTE-NO-CACHE--"
  puts out2.include?("vendor/cache")
  puts $?.exitstatus
end

ENV["GEM_HOME"] = old_gem_home
ENV["HOME"]     = old_home
system("rm", "-rf", root)
