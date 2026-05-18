$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_bundle_remove_#{$$}"
app      = File.join(root, "app")
gem1_dir = File.join(root, "gem1")
gem2_dir = File.join(root, "gem2")
[root, app, gem1_dir, gem2_dir,
 File.join(gem1_dir, "lib"), File.join(gem2_dir, "lib")].each do |d|
  Dir.mkdir(d) unless Dir.exist?(d)
end

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

make_path_gem(gem1_dir, "gem1")
make_path_gem(gem2_dir, "gem2")

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "gem1", path: #{gem1_dir.inspect}
  gem "gem2", path: #{gem2_dir.inspect}
GEMFILE

def capture(command)
  output = `#{command} 2>&1`.chomp
  [output, $?.exitstatus]
end

bundle_exe = File.expand_path("exe/bundle", Dir.pwd)
ruby_cmd   = RbConfig.ruby.inspect

Dir.chdir(app) do
  `#{ruby_cmd} #{bundle_exe.inspect} install 2>&1`

  puts "--LIST-BEFORE--"
  puts `#{ruby_cmd} #{bundle_exe.inspect} list 2>&1`.chomp

  # remove gem1
  puts "--REMOVE--"
  out, status = capture("#{ruby_cmd} #{bundle_exe.inspect} remove gem1")
  puts out
  puts status

  # gem1 is gone from Gemfile
  puts "--GEMFILE--"
  puts File.read("Gemfile").chomp

  # gem2 still there
  puts "--LIST-AFTER--"
  puts `#{ruby_cmd} #{bundle_exe.inspect} list 2>&1`.chomp

  # removing gem1 again is an error
  puts "--REMOVE-AGAIN--"
  out, status = capture("#{ruby_cmd} #{bundle_exe.inspect} remove gem1")
  puts out
  puts status

  # no gem name
  puts "--NO-NAME--"
  out, status = capture("#{ruby_cmd} #{bundle_exe.inspect} remove")
  puts out
  puts status
end

system("rm", "-rf", root)
