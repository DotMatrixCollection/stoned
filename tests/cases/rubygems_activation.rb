require "rubygems"

root = "/tmp/stoned_rubygems_activation_#{$$}"
gems_root = File.join(root, "gems")
demo_root = File.join(gems_root, "demo-1.2.3")
lib_dir = File.join(demo_root, "lib")
spec_dir = File.join(root, "specifications")

[root, gems_root, demo_root, lib_dir, spec_dir].each do |dir|
  Dir.mkdir(dir) unless Dir.exist?(dir)
end

File.write(File.join(lib_dir, "demo.rb"), "DEMO_VALUE = 42\n")
File.write(File.join(spec_dir, "demo-1.2.3.gemspec"), <<~GEMSPEC)
  Gem::Specification.new do |s|
    s.name = "demo"
    s.version = Gem::Version.new("1.2.3")
    s.require_paths = ["lib"]
  end
GEMSPEC

ENV["GEM_HOME"] = root
Gem.clear_paths
Gem::Specification.reset!

spec = Gem::Specification.find_by_name("demo")
puts spec.full_name
puts gem("demo")
puts Gem.loaded_specs["demo"].full_name
require "demo"
puts DEMO_VALUE

system("rm", "-rf", root)
