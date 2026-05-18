$stdout.sync = true

require "rbconfig"

root     = "/tmp/stoned_bundle_install_path_#{$$}"
app      = File.join(root, "app")
gem_dir  = File.join(root, "mygem")
lib_dir  = File.join(gem_dir, "lib")
[root, app, gem_dir, lib_dir].each do |d|
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

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "mygem", path: #{gem_dir.inspect}
GEMFILE

old_home = ENV["HOME"]
ENV["HOME"] = root

ruby_cmd   = RbConfig.ruby.inspect
bundle_exe = File.expand_path("exe/bundle", Dir.pwd)

Dir.chdir(app) do
  # Install to vendor/bundle via --path flag
  out = `#{ruby_cmd} #{bundle_exe.inspect} install --path vendor/bundle 2>&1`.chomp
  puts "--INSTALL--"
  puts out.include?("Bundle complete!")

  # Config should persist BUNDLE_PATH
  cfg = File.read(".bundle/config") rescue ""
  puts "--CONFIG--"
  puts cfg.include?("BUNDLE_PATH")
  puts cfg.include?("vendor/bundle")

  # Second install with path set in config should reuse it
  out2 = `#{ruby_cmd} #{bundle_exe.inspect} install 2>&1`.chomp
  puts "--SECOND--"
  puts out2.include?("Bundle") || out2.include?("up to date")
end

ENV["HOME"] = old_home
system("rm", "-rf", root)
