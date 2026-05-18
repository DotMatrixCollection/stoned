$stdout.sync = true

require "rbconfig"

root = "/tmp/stoned_bundle_add_#{$$}"
app      = File.join(root, "app")
gem1_dir = File.join(root, "gem1")
gem2_dir = File.join(root, "gem2")
gem3_dir = File.join(root, "gem3")
[root, app, gem1_dir, gem2_dir, gem3_dir,
 File.join(gem1_dir, "lib"), File.join(gem2_dir, "lib"), File.join(gem3_dir, "lib")].each do |d|
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
make_path_gem(gem3_dir, "gem3")

File.write(File.join(app, "Gemfile"), <<~GEMFILE)
  source "https://rubygems.org"
  gem "gem1", path: #{gem1_dir.inspect}
GEMFILE

def capture(command)
  output = `#{command} 2>&1`.chomp
  [output, $?.exitstatus]
end

bundle_exe = File.expand_path("exe/bundle", Dir.pwd)
ruby_cmd   = RbConfig.ruby.inspect

Dir.chdir(app) do
  # Initial install
  `#{ruby_cmd} #{bundle_exe.inspect} install 2>&1`

  # add gem2
  puts "--ADD--"
  out, status = capture("#{ruby_cmd} #{bundle_exe.inspect} add gem2 --path #{gem2_dir.inspect}")
  puts out
  puts status

  # Gemfile now has gem2
  puts "--GEMFILE-HAS-GEM2--"
  puts File.read("Gemfile").include?("gem2")

  # lockfile updated
  puts "--LOCK--"
  puts File.read("Gemfile.lock").chomp

  # add with group
  puts "--ADD-GROUP--"
  out, status = capture("#{ruby_cmd} #{bundle_exe.inspect} add gem3 --path #{gem3_dir.inspect} --group development")
  puts out
  puts status

  # adding gem1 again is an error
  puts "--DUPLICATE--"
  out, status = capture("#{ruby_cmd} #{bundle_exe.inspect} add gem1 --path #{gem1_dir.inspect}")
  puts out
  puts status

  # add without gem name is an error
  puts "--NO-NAME--"
  out, status = capture("#{ruby_cmd} #{bundle_exe.inspect} add")
  puts out
  puts status
end

system("rm", "-rf", root)
