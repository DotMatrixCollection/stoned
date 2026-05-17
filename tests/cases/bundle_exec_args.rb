$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_bundle_exec_args_#{$$}"
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

File.write(File.join(lib_dir, "demo.rb"), "module Demo\nend\n")

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "demo", path: #{gem_root.inspect}
GEMFILE

File.write(File.join(app, "show.rb"), "puts ARGV.inspect\n")

bundle_exe = File.expand_path("exe/bundle", Dir.pwd)

Dir.chdir(app) do
  puts `#{RbConfig.ruby} #{bundle_exe} install`.chomp
  puts `#{RbConfig.ruby} #{bundle_exe} exec show.rb alpha beta`.chomp
end

system("rm", "-rf", root)
