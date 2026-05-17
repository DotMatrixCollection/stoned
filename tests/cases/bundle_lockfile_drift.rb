$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_bundle_lockfile_drift_#{$$}"
app = File.join(root, "app")
demo_root = File.join(root, "demo_gem")
extra_root = File.join(root, "extra_gem")
demo_lib = File.join(demo_root, "lib")
extra_lib = File.join(extra_root, "lib")
[root, app, demo_root, extra_root, demo_lib, extra_lib].each do |dir|
  Dir.mkdir(dir) unless Dir.exist?(dir)
end

File.write(File.join(demo_root, "demo.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "demo"
    s.version = Gem::Version.new("0.5.0")
    s.summary = "demo"
    s.files = ["lib/demo.rb"]
    s.require_paths = ["lib"]
  end
GEMSPEC

File.write(File.join(extra_root, "extra.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "extra"
    s.version = Gem::Version.new("1.0.0")
    s.summary = "extra"
    s.files = ["lib/extra.rb"]
    s.require_paths = ["lib"]
  end
GEMSPEC

File.write(File.join(demo_lib, "demo.rb"), "module Demo\nend\n")
File.write(File.join(extra_lib, "extra.rb"), "module Extra\nend\n")

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "demo", path: #{demo_root.inspect}
GEMFILE

bundle_exe = File.expand_path("exe/bundle", Dir.pwd)

Dir.chdir(app) do
  puts `#{RbConfig.ruby} #{bundle_exe} install`.chomp
  puts "--LOCK1--"
  puts File.read("Gemfile.lock").chomp
  puts "--LIST1--"
  puts `#{RbConfig.ruby} #{bundle_exe} list`.chomp
  puts "--CHECK1--"
  puts `#{RbConfig.ruby} #{bundle_exe} check`.chomp

  File.write("Gemfile", <<~GEMFILE)
    source "https://rubygems.org"
    gem "demo", path: #{demo_root.inspect}
    gem "extra", path: #{extra_root.inspect}
  GEMFILE

  puts "--LIST2--"
  puts `#{RbConfig.ruby} #{bundle_exe} list`.chomp
  puts "--CHECK2--"
  begin
    puts `#{RbConfig.ruby} #{bundle_exe} check 2>&1`.chomp
  rescue
  end

  puts "--INSTALL2--"
  puts `#{RbConfig.ruby} #{bundle_exe} install`.chomp
  puts "--LOCK2--"
  puts File.read("Gemfile.lock").chomp
  puts "--CHECK3--"
  puts `#{RbConfig.ruby} #{bundle_exe} check`.chomp
end

system("rm", "-rf", root)
