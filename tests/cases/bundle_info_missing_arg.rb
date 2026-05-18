$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_bundle_info_missing_arg_#{$$}"
app = File.join(root, "app")
dep_root = File.join(root, "demo")
dep_lib = File.join(dep_root, "lib")
[root, app, dep_root, dep_lib].each do |dir|
  Dir.mkdir(dir) unless Dir.exist?(dir)
end

File.write(File.join(dep_root, "demo.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "demo"
    s.version = Gem::Version.new("1.2.3")
    s.summary = "demo"
    s.files = ["lib/demo.rb"]
    s.require_paths = ["lib"]
  end
GEMSPEC

File.write(File.join(dep_lib, "demo.rb"), "module Demo\nend\n")

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "demo", path: #{dep_root.inspect}
GEMFILE

def capture(command)
  output = `#{command} 2>&1`.chomp
  [output, $?.exitstatus]
end

bundle_exe = File.expand_path("exe/bundle", Dir.pwd)

Dir.chdir(app) do
  install_out, = capture("#{RbConfig.ruby.inspect} #{bundle_exe.inspect} install")
  puts install_out

  out, status = capture("#{RbConfig.ruby.inspect} #{bundle_exe.inspect} info")
  puts out
  puts status
end

system("rm", "-rf", root)
