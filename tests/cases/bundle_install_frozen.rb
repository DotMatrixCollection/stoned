$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_bundle_install_frozen_#{$$}"
app = File.join(root, "app")
gem_root = File.join(root, "demo_gem")
lib_dir = File.join(gem_root, "lib")
[root, app, gem_root, lib_dir].each do |dir|
  Dir.mkdir(dir) unless Dir.exist?(dir)
end

File.write(File.join(gem_root, "demo.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "demo"
    s.version = Gem::Version.new("1.0.0")
    s.summary = "demo"
    s.files = ["lib/demo.rb"]
    s.require_paths = ["lib"]
  end
GEMSPEC

File.write(File.join(lib_dir, "demo.rb"), "module Demo\nend\n")

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "demo", path: #{gem_root.inspect}
GEMFILE

def capture(command)
  output = `#{command} 2>&1`.chomp
  [output, $?.exitstatus]
end

bundle_exe = File.expand_path("exe/bundle", Dir.pwd)
ruby_cmd = RbConfig.ruby.inspect

Dir.chdir(app) do
  # --frozen with no lockfile should fail
  puts "--FROZEN-NO-LOCK--"
  out, status = capture("#{ruby_cmd} #{bundle_exe.inspect} install --frozen")
  puts out
  puts status

  # normal install to create lockfile
  `#{ruby_cmd} #{bundle_exe.inspect} install 2>&1`

  # --frozen with matching lockfile should succeed
  puts "--FROZEN-OK--"
  out, status = capture("#{ruby_cmd} #{bundle_exe.inspect} install --frozen")
  puts out
  puts status

  # modify Gemfile to add a new dep (drift)
  extra_root = File.join(root, "extra_gem")
  extra_lib  = File.join(extra_root, "lib")
  Dir.mkdir(extra_root)
  Dir.mkdir(extra_lib)
  File.write(File.join(extra_root, "extra.gemspec"), <<~GEMSPEC)
    Gem::Specification.new do |s|
      s.name = "extra"
      s.version = Gem::Version.new("0.1.0")
      s.summary = "extra"
      s.files = ["lib/extra.rb"]
      s.require_paths = ["lib"]
    end
  GEMSPEC
  File.write(File.join(extra_lib, "extra.rb"), "module Extra; end\n")
  File.write("Gemfile", <<~GEMFILE)
    source "https://rubygems.org"
    gem "demo",  path: #{gem_root.inspect}
    gem "extra", path: #{extra_root.inspect}
  GEMFILE

  # --frozen with drifted Gemfile should fail
  puts "--FROZEN-DRIFT--"
  out, status = capture("#{ruby_cmd} #{bundle_exe.inspect} install --frozen")
  puts out
  puts status
end

system("rm", "-rf", root)
