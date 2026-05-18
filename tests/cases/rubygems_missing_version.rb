require "rubygems"

root = "/tmp/stoned_rubygems_missing_version_#{$$}"
gems_root = File.join(root, "gems")
spec_dir = File.join(root, "specifications")

[root, gems_root, spec_dir].each do |dir|
  Dir.mkdir(dir) unless Dir.exist?(dir)
end

["1.2.3", "2.0.0"].each do |version|
  demo_root = File.join(gems_root, "demo-#{version}")
  lib_dir = File.join(demo_root, "lib")
  [demo_root, lib_dir].each do |dir|
    Dir.mkdir(dir) unless Dir.exist?(dir)
  end

  File.write(File.join(lib_dir, "demo.rb"), "DEMO_VALUE = #{version == "1.2.3" ? 123 : 200}\n")
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

puts Gem.available?("demo")
puts Gem.available?("demo", "= 1.2.3")
puts Gem.available?("demo", "= 9.9.9")
puts Gem.available?("missing")

begin
  gem("demo", "= 9.9.9")
rescue => e
  puts e.class.to_s
  puts e.message
end

begin
  gem("missing", "= 1.0.0")
rescue => e
  puts e.class.to_s
  puts e.message
end

system("rm", "-rf", root)
