require "bundler"
require File.join(BUNDLER_RB_DIR, "bundler", "lazy_specification")

module Bundler
  class RemoteSpecification < LazySpecification
  end
end
