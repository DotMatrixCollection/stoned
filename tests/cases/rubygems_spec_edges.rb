require "rubygems"

spec = Gem::Specification.new do |s|
  s.name = "demo"
  s.version = Gem::Version.new("0.1.0")
  s.summary = "Demo"
  s.license = "MIT"
  s.add_runtime_dependency "dep", ">= 1"
end

p spec.full_name
p spec.licenses
p spec.runtime_dependencies.first.name
p spec.to_s
