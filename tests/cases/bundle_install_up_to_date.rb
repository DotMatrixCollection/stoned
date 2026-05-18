$stdout.sync = true

require "rbconfig"

# Verifies "Bundle up to date!" appears when lockfile is already current.

root = "/tmp/stoned_bundle_install_up_to_date_#{$$}"
app      = File.join(root, "app")
gem_dir  = File.join(root, "gem")
[root, app, gem_dir, File.join(gem_dir, "lib")].each { |d| Dir.mkdir(d) unless Dir.exist?(d) }

old_home = ENV["HOME"]
ENV["HOME"] = root

File.write(File.join(gem_dir, "mygem.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "mygem"
    s.version = Gem::Version.new("1.0.0")
    s.summary = "mygem"
    s.files = ["lib/mygem.rb"]
    s.require_paths = ["lib"]
  end
GEMSPEC
File.write(File.join(gem_dir, "lib", "mygem.rb"), "module Mygem; end\n")

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "mygem", path: #{gem_dir.inspect}
GEMFILE

bundle_exe = File.expand_path("exe/bundle", Dir.pwd)
ruby_cmd   = RbConfig.ruby.inspect

Dir.chdir(app) do
  # First install: lockfile created, "Bundle complete!"
  puts "--FIRST-INSTALL--"
  puts `#{ruby_cmd} #{bundle_exe.inspect} install 2>&1`.chomp

  # Second install: lockfile unchanged, "Bundle up to date!"
  puts "--SECOND-INSTALL--"
  puts `#{ruby_cmd} #{bundle_exe.inspect} install 2>&1`.chomp
end

ENV["HOME"] = old_home
system("rm", "-rf", root)
