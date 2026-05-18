require "rubygems"

root = "/tmp/stoned_rubygems_activation_requirements_#{$$}"
gems_root = File.join(root, "gems")
spec_dir = File.join(root, "specifications")

[root, gems_root, spec_dir].each do |dir|
  Dir.mkdir(dir) unless Dir.exist?(dir)
end

[
  ["1.2.3", "DEMO_VALUE = 123\n"],
  ["2.0.0", "DEMO_VALUE = 200\n"],
].each do |version, lib_body|
  demo_root = File.join(gems_root, "demo-#{version}")
  lib_dir = File.join(demo_root, "lib")
  [demo_root, lib_dir].each do |dir|
    Dir.mkdir(dir) unless Dir.exist?(dir)
  end

  File.write(File.join(lib_dir, "demo.rb"), lib_body)
  File.write(File.join(spec_dir, "demo-#{version}.gemspec"), <<~GEMSPEC)
    Gem::Specification.new do |s|
      s.name = "demo"
      s.version = Gem::Version.new(#{version.inspect})
      s.require_paths = ["lib"]
    end
  GEMSPEC
end

ENV["GEM_HOME"] = root
Gem.clear_paths
Gem::Specification.reset!

puts Gem::Specification.find_by_name("demo").full_name
puts Gem::Specification.find_by_name("demo", "= 1.2.3").full_name
puts Gem::Specification.find_all_by_name("demo", ">= 1.0").map(&:full_name).inspect
puts gem("demo", "= 1.2.3")
puts Gem.loaded_specs["demo"].full_name
require "demo"
puts DEMO_VALUE

system("rm", "-rf", root)
