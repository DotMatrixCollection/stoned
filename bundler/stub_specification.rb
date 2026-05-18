require "bundler"
require File.join(BUNDLER_RB_DIR, "bundler", "lazy_specification")

module Bundler
  class StubSpecification < LazySpecification
  end
end
