require "bundler"

module Bundler
  class SpecSet
    def initialize(specs = [])
      @specs = specs
    end

    def [](index)
      @specs[index]
    end

    def each(&block)
      @specs.each(&block)
    end

    def length
      @specs.length
    end

    def size
      length
    end

    def empty?
      @specs.empty?
    end

    def to_a
      @specs
    end
  end
end
