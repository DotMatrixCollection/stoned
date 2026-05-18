$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_bundle_install_without_flag_#{$$}"
app      = File.join(root, "app")
core_dir = File.join(root, "core_gem")
dev_dir  = File.join(root, "dev_gem")
test_dir = File.join(root, "test_gem")
[root, app, core_dir, dev_dir, test_dir,
 File.join(core_dir, "lib"), File.join(dev_dir, "lib"), File.join(test_dir, "lib")].each do |d|
  Dir.mkdir(d) unless Dir.exist?(d)
end

old_home = ENV["HOME"]
ENV["HOME"] = root  # isolate global bundle config

def make_path_gem(dir, name)
  File.write(File.join(dir, "#{name}.gemspec"), <<~GEMSPEC)
    Gem::Specification.new do |s|
      s.name = "#{name}"
      s.version = Gem::Version.new("1.0.0")
      s.summary = "#{name}"
      s.files = ["lib/#{name}.rb"]
      s.require_paths = ["lib"]
    end
  GEMSPEC
  File.write(File.join(dir, "lib", "#{name}.rb"), "module #{name.capitalize}; end\n")
end

make_path_gem(core_dir, "core")
make_path_gem(dev_dir,  "devgem")
make_path_gem(test_dir, "testgem")

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "core", path: #{core_dir.inspect}
  group :development do
    gem "devgem", path: #{dev_dir.inspect}
  end
  group :test do
    gem "testgem", path: #{test_dir.inspect}
  end
GEMFILE

def capture(command)
  output = `#{command} 2>&1`.chomp
  [output, $?.exitstatus]
end

bundle_exe = File.expand_path("exe/bundle", Dir.pwd)
ruby_cmd   = RbConfig.ruby.inspect

Dir.chdir(app) do
  # install --without development:test persists to config
  out, status = capture("#{ruby_cmd} #{bundle_exe.inspect} install --without development:test")
  puts out
  puts status

  # list reflects the WITHOUT setting (persisted in .bundle/config)
  puts "--LIST--"
  puts `#{ruby_cmd} #{bundle_exe.inspect} list 2>&1`.chomp

  # config shows the persisted value
  puts "--CONFIG--"
  puts `#{ruby_cmd} #{bundle_exe.inspect} config get WITHOUT 2>&1`.chomp

  # install again without the flag still respects the persisted config
  puts "--REINSTALL--"
  `#{ruby_cmd} #{bundle_exe.inspect} install 2>&1`
  puts `#{ruby_cmd} #{bundle_exe.inspect} list 2>&1`.chomp
end

ENV["HOME"] = old_home
system("rm", "-rf", root)
