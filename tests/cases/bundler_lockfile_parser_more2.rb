require "bundler/lockfile_parser"

lock = "GEM\n  specs:\n    demo (1.0.0)\n\nDEPENDENCIES\n  demo\n"
p = Bundler::LockfileParser.new(lock)
p p.specs.map(&:name)
p p.dependencies
