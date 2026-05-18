$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_bundle_binstubs_#{$$}"
app = File.join(root, "app")
gem_root = File.join(root, "demo_gem")
lib_dir  = File.join(gem_root, "lib")
bin_dir  = File.join(gem_root, "bin")
[root, app, gem_root, lib_dir, bin_dir].each do |dir|
  Dir.mkdir(dir) unless Dir.exist?(dir)
end

File.write(File.join(gem_root, "demo.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "demo"
    s.version = Gem::Version.new("0.7.0")
    s.summary = "demo"
    s.files = ["lib/demo.rb", "bin/demo-run"]
    s.require_paths = ["lib"]
    s.executables = ["demo-run"]
    s.bindir = "bin"
  end
GEMSPEC

File.write(File.join(lib_dir, "demo.rb"), "module Demo\n  VALUE = 77\nend\n")
File.write(File.join(bin_dir, "demo-run"), <<~RUBY)
  require "demo"
  puts Demo::VALUE
RUBY

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "demo", path: #{gem_root.inspect}
GEMFILE

bundle_exe = File.expand_path("exe/bundle", Dir.pwd)
ruby_cmd   = RbConfig.ruby.inspect

Dir.chdir(app) do
  puts `#{ruby_cmd} #{bundle_exe.inspect} install 2>&1`.chomp

  puts "--BINSTUBS--"
  puts `#{ruby_cmd} #{bundle_exe.inspect} binstubs demo 2>&1`.chomp

  puts "--STUB-EXISTS--"
  puts File.exist?("bin/demo-run")

  puts "--STUB-CONTENTS--"
  puts File.read("bin/demo-run").chomp

  puts "--MISSING-GEM--"
  out = `#{ruby_cmd} #{bundle_exe.inspect} binstubs nosuchgem 2>&1`.chomp
  puts out

  puts "--NO-EXEC-GEM--"
  dep_root = File.join(root, "nodep_gem")
  dep_lib  = File.join(dep_root, "lib")
  Dir.mkdir(dep_root) unless Dir.exist?(dep_root)
  Dir.mkdir(dep_lib)  unless Dir.exist?(dep_lib)
  File.write(File.join(dep_root, "nodep.gemspec"), <<~GEMSPEC)
    Gem::Specification.new do |s|
      s.name = "nodep"
      s.version = Gem::Version.new("0.1.0")
      s.summary = "nodep"
      s.files = ["lib/nodep.rb"]
      s.require_paths = ["lib"]
    end
  GEMSPEC
  File.write(File.join(dep_lib, "nodep.rb"), "module Nodep; end\n")
  File.write("Gemfile", <<~GEMFILE)
    source "https://rubygems.org"
    gem "demo",  path: #{gem_root.inspect}
    gem "nodep", path: #{dep_root.inspect}
  GEMFILE
  `#{ruby_cmd} #{bundle_exe.inspect} install 2>&1`
  puts `#{ruby_cmd} #{bundle_exe.inspect} binstubs nodep 2>&1`.chomp
end

system("rm", "-rf", root)
