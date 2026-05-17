require "rbconfig"

root = "/tmp/stoned_bundle_path_exec_#{$$}"
app = File.join(root, "app")
gem_root = File.join(root, "demo_gem")
lib_dir = File.join(gem_root, "lib")
[root, app, gem_root, lib_dir].each do |dir|
  Dir.mkdir(dir) unless Dir.exist?(dir)
end

File.write(File.join(gem_root, "demo.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "demo"
    s.version = Gem::Version.new("0.5.0")
    s.summary = "demo"
    s.files = ["lib/demo.rb"]
    s.require_paths = ["lib"]
  end
GEMSPEC

File.write(File.join(lib_dir, "demo.rb"), <<~RUBY)
  module Demo
    VALUE = 99
  end
RUBY

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "demo", path: #{gem_root.inspect}
GEMFILE

File.write(File.join(app, "show.rb"), <<~RUBY)
  require "demo"
  puts Demo::VALUE
RUBY

bundle_exe = File.expand_path("exe/bundle", Dir.pwd)

Dir.chdir(app) do
  puts system(RbConfig.ruby, bundle_exe, "install")
  puts File.exist?(File.join(app, "Gemfile.lock"))
  puts system(RbConfig.ruby, bundle_exe, "check")
  puts system(RbConfig.ruby, bundle_exe, "exec", "show.rb")
end

system("rm", "-rf", root)
